<xsl:stylesheet version = '1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:sql="http://www.gnome-db.org/ns/gda-sql-ext-v4"
     extension-element-prefixes="sql">

<xsl:output method="html" indent="yes" encoding="utf-8"/>

<xsl:template match="/">
<!-- The start template -->
<!-- now only one connection is posible to use -->
<h1>
<xsl:for-each select="//saleid">
  <xsl:call-template name="showsale"/>
</xsl:for-each>
</h1>
</xsl:template>

<xsl:template name="showsale">
<xsl:variable name="srep" select="text()"/>
<sql:section>
  <!-- no predefined query are registred, so use the sql text inside the node -->
  <sql:query name="customers">
  select * from customers
  where
  default_served_by = ##srep::gint
  </sql:query>
  <!-- $srep is set on xsl:variable at begin -->
  <sql:template>
  <!-- !!! sql:template allows only
        ___ONE___ xsl:call_template element (optionally with <xsl:params.. inside)
  -->
  <xsl:call-template name="customerData"/>
  </sql:template>
</sql:section>
  <!-- not implemented, but here the query 'user' will not be avalible-->
</xsl:template>

<xsl:template name="customerData">
<ul>
  <li>Id: <xsl:value-of select="sql:getvalue('customers','id')"/></li>
  <li>Name: <xsl:value-of select="sql:getvalue('customers','name')"/></li>
</ul>
</xsl:template>

</xsl:stylesheet>
