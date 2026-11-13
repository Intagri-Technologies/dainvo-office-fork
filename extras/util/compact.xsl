<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0">

  <xsl:param name="dainvo-version"/>

  <xsl:template match="meta:generator">
    <xsl:copy>
      <xsl:choose>
        <xsl:when test="$dainvo-version != ''">
          <xsl:text>Dainvo Office </xsl:text>
          <xsl:value-of select="$dainvo-version"/>
        </xsl:when>
        <xsl:otherwise><xsl:apply-templates select="@*|node()"/></xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
  </xsl:template>

<xsl:output method="xml" omit-xml-declaration="yes"/>

  <xsl:strip-space elements="*"/>

  <xsl:template match="@*|node()">
   <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
   </xsl:copy>
  </xsl:template>

</xsl:stylesheet>